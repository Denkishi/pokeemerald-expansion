// =============================================================================
// Custom Team Builder
// Allows the player to build a custom team of 1-6 Pokemon from scratch.
// =============================================================================

#include "global.h"
#include "main.h"
#include "menu.h"
#include "menu_helpers.h"
#include "palette.h"
#include "task.h"
#include "bg.h"
#include "gpu_regs.h"
#include "window.h"
#include "text.h"
#include "text_window.h"
#include "string_util.h"
#include "malloc.h"
#include "pokemon.h"
#include "pokemon_icon.h"
#include "item.h"
#include "move.h"
#include "sound.h"
#include "scanline_effect.h"
#include "script.h"
#include "overworld.h"
#include "event_data.h"
#include "field_screen_effect.h"
#include "battle_main.h"
#include "script_pokemon_util.h"
#include "custom_team_builder.h"
#include "constants/songs.h"
#include "constants/rgb.h"
#include "constants/pokemon.h"
#include "constants/items.h"
#include "constants/moves.h"
#include "constants/global.h"
#include "constants/abilities.h"


// ---------------------------------------------------------------------------
// Screen states
// ---------------------------------------------------------------------------
enum {
    SCREEN_TEAM_OVERVIEW,
    SCREEN_SELECT_SPECIES,
    SCREEN_SELECT_MOVES,
    SCREEN_SELECT_NATURE,
    SCREEN_SELECT_ABILITY,
    SCREEN_SELECT_ITEM,
    SCREEN_SELECT_TERA,
    SCREEN_SET_EVS,
    SCREEN_SET_IVS,
    SCREEN_CONFIRM_MON,
    SCREEN_CONFIRM_TEAM,
    SCREEN_FADE_OUT,
};

// ---------------------------------------------------------------------------
// Window IDs
// ---------------------------------------------------------------------------
enum {
    WIN_HEADER,
    WIN_LEFT,
    WIN_RIGHT,
    WIN_COUNT
};

// ---------------------------------------------------------------------------
// Task data indices (u16 shorts in gTasks[taskId].data[])
// ---------------------------------------------------------------------------
#define TD_PREV_SCROLL   0   // saved scroll for return navigation

// ---------------------------------------------------------------------------
// Limits
// ---------------------------------------------------------------------------
#define MAX_LEARNSET_MOVES   96
#define MAX_SPECIES_LIST    1050
#define MAX_ITEM_LIST       1050
#define EV_MAX_PER_STAT     252
#define EV_MAX_TOTAL        510
#define MAX_TEAM_SIZE       6
#define PARTY_SIZE          6
#define ICON_SPRITE_NONE    0xFF

// ---------------------------------------------------------------------------
// Data structures
// ---------------------------------------------------------------------------
struct CustomBuilderMon {
    u16 species;
    u16 moves[MAX_MON_MOVES];
    u8  nature;
    u8  abilityNum;
    u16 item;
    u8  teraType;
    u16 evs[NUM_STATS];
    u8  ivs[NUM_STATS];
};

struct CustomBuilderData {
    u8  screen;
    u8  teamCount;
    u8  curTeamSlot;   // which party slot is being edited (0-5)
    u8  curMoveSlot;   // which move slot is being chosen (0-3)
    u8  curStatIdx;    // which stat is highlighted for EV/IV
    u8  filterLetter;  // 0 = no filter, 1-26 = A-Z
    u16 listCursor;    // cursor within filtered list
    u16 listScroll;    // scroll offset in filtered list
    struct CustomBuilderMon team[MAX_TEAM_SIZE];

    // Filtered index arrays
    u16 speciesList[MAX_SPECIES_LIST];
    u16 speciesCount;
    u16 moveList[MAX_LEARNSET_MOVES];
    u16 moveCount;
    u16 itemList[MAX_ITEM_LIST];
    u16 itemCount;

    // Sprite IDs
    u8 iconSpriteId;   // single mon icon in right panel
    u8 teamIconIds[MAX_TEAM_SIZE]; // icons in team overview

    // Window IDs
    u8 winIds[WIN_COUNT];
};

EWRAM_DATA static struct CustomBuilderData *sData = NULL;

// ---------------------------------------------------------------------------
// Static strings
// ---------------------------------------------------------------------------
static const u8 sText_Header_Overview[]  = _("TEAM CUSTOM");
static const u8 sText_Header_Species[]   = _("SCEGLI POKEMON");
static const u8 sText_Header_Moves[]     = _("SCEGLI MOSSE");
static const u8 sText_Header_Nature[]    = _("SCEGLI NATURA");
static const u8 sText_Header_Ability[]   = _("SCEGLI ABILITA");
static const u8 sText_Header_Item[]      = _("SCEGLI OGGETTO");
static const u8 sText_Header_Tera[]      = _("TIPO TERA");
static const u8 sText_Header_EVs[]       = _("SETT. EV");
static const u8 sText_Header_IVs[]       = _("SETT. IV");

static const u8 sText_AddPokemon[]       = _("+ Aggiungi Pokemon");
static const u8 sText_Confirm[]          = _("CONFERMA TEAM");
static const u8 sText_Cancel[]           = _("Annulla");
static const u8 sText_Empty[]            = _("---");
static const u8 sText_Nessuno[]          = _("Nessuno");
static const u8 sText_PressStart[]       = _("START:Conferma  B:Esci");
static const u8 sText_PressA[]           = _("A:Scegli  B:Torna");
static const u8 sText_PressAB[]          = _("A:Conf  B:Torna");
static const u8 sText_LR[]              = _("{L_BUTTON}/{R_BUTTON}:Cambia  B:OK");
static const u8 sText_Stat_HP[]          = _("PS");
static const u8 sText_Stat_Atk[]         = _("Att");
static const u8 sText_Stat_Def[]         = _("Dif");
static const u8 sText_Stat_SpA[]         = _("AtS");
static const u8 sText_Stat_SpD[]         = _("DiS");
static const u8 sText_Stat_Spe[]         = _("Vel");
static const u8 sText_Up[]               = _("+");
static const u8 sText_Down[]             = _("-");
static const u8 sText_Neutral[]          = _("=");
static const u8 sText_Slash[]            = _("/");
static const u8 sText_Colon[]            = _(":");
static const u8 sText_Space[]            = _(" ");
static const u8 sText_TotalEV[]          = _("Tot EV:");
static const u8 sText_Lv50[]             = _("Lv.50");
static const u8 sText_Filter[]           = _("Filtro:");
static const u8 sText_HiddenAbil[]       = _("[H]");

static const u8 *const sStatNames[NUM_STATS] = {
    sText_Stat_HP,
    sText_Stat_Atk,
    sText_Stat_Def,
    sText_Stat_Spe,
    sText_Stat_SpA,
    sText_Stat_SpD,
};

// Forward declarations
static void Task_CTB_Main(u8 taskId);
static void CTB_DrawScreen(void);
static void CTB_DrawOverview(void);
static void CTB_DrawSpeciesList(void);
static void CTB_DrawMoveList(void);
static void CTB_DrawNatureList(void);
static void CTB_DrawAbilityList(void);
static void CTB_DrawItemList(void);
static void CTB_DrawTeraList(void);
static void CTB_DrawEVsScreen(void);
static void CTB_DrawIVsScreen(void);
static void CTB_ClearIcon(void);
static void CTB_ShowMonIcon(u16 species);
static void CTB_PopulateSpecies(void);
static void CTB_PopulateMoves(u16 species);
static void CTB_PopulateItems(void);
static void CTB_ClampCursor(void);
static void CTB_BuildAndExitTeam(u8 taskId);
static void CTB_ScrolledLine(u8 winId, u8 fontId, const u8 *str, u8 lineIdx, bool8 selected);
static void CB2_CTB_Main(void);

// ---------------------------------------------------------------------------
// BG / Window templates (same layout as rental_teams.c)
// ---------------------------------------------------------------------------
static const struct BgTemplate sBgTemplates[] = {
    {
        .bg           = 0,
        .charBaseIndex = 0,
        .mapBaseIndex  = 31,
        .screenSize    = 0,
        .paletteMode   = 0,
        .priority      = 0,
        .baseTile      = 0,
    },
};

static const struct WindowTemplate sWinTemplates[WIN_COUNT + 1] = {
    [WIN_HEADER] = {
        .bg          = 0,
        .tilemapLeft = 0,
        .tilemapTop  = 0,
        .width       = 30,
        .height      = 3,
        .paletteNum  = 15,
        .baseBlock   = 1,
    },
    [WIN_LEFT] = {
        .bg          = 0,
        .tilemapLeft = 0,
        .tilemapTop  = 3,
        .width       = 17,
        .height      = 15,
        .paletteNum  = 15,
        .baseBlock   = 1 + 30 * 3,
    },
    [WIN_RIGHT] = {
        .bg          = 0,
        .tilemapLeft = 17,
        .tilemapTop  = 3,
        .width       = 13,
        .height      = 15,
        .paletteNum  = 15,
        .baseBlock   = 1 + 30 * 3 + 17 * 15,
    },
    DUMMY_WIN_TEMPLATE,
};

// ---------------------------------------------------------------------------
// VBlank callback
// ---------------------------------------------------------------------------
static void VBlankCB_CTB(void)
{
    LoadOam();
    ProcessSpriteCopyRequests();
    TransferPlttBuffer();
}

// ---------------------------------------------------------------------------
// Entry point called from script
// ---------------------------------------------------------------------------
void OpenCustomTeamBuilder(void)
{
    u8 taskId;
    u8 i;

    // Allocate state
    sData = AllocZeroed(sizeof(struct CustomBuilderData));
    if (sData == NULL)
        return;

    sData->screen       = SCREEN_TEAM_OVERVIEW;
    sData->teamCount    = 0;
    sData->iconSpriteId = ICON_SPRITE_NONE;
    for (i = 0; i < MAX_TEAM_SIZE; i++)
    {
        sData->teamIconIds[i] = ICON_SPRITE_NONE;
        // Default IVs to 31
        {
            u8 s;
            for (s = 0; s < NUM_STATS; s++)
                sData->team[i].ivs[s] = 31;
        }
    }

    // Setup GBA hardware
    SetVBlankCallback(NULL);
    ResetVramOamAndBgCntRegs();
    ResetBgsAndClearDma3BusyFlags(TRUE);
    InitBgsFromTemplates(0, sBgTemplates, ARRAY_COUNT(sBgTemplates));
    ResetAllBgsCoordinates();

    // Load palettes
    FillBgTilemapBufferRect_Palette0(0, 0, 0, 0, 30, 20, 0);
    LoadUserWindowBorderGfx(0, 0x0200, BG_PLTT_ID(14));
    LoadPalette(gStandardMenuPalette, BG_PLTT_ID(15), PLTT_SIZEOF(1));

    // Load mon icon palettes
    LoadMonIconPalettes();

    // Create windows
    sData->winIds[WIN_HEADER] = AddWindow(&sWinTemplates[WIN_HEADER]);
    sData->winIds[WIN_LEFT]   = AddWindow(&sWinTemplates[WIN_LEFT]);
    sData->winIds[WIN_RIGHT]  = AddWindow(&sWinTemplates[WIN_RIGHT]);

    // Draw frames
    DrawStdFrameWithCustomTileAndPalette(sData->winIds[WIN_HEADER], FALSE, 0x0200, 14);
    DrawStdFrameWithCustomTileAndPalette(sData->winIds[WIN_LEFT],   FALSE, 0x0200, 14);
    DrawStdFrameWithCustomTileAndPalette(sData->winIds[WIN_RIGHT],  FALSE, 0x0200, 14);

    PutWindowTilemap(sData->winIds[WIN_HEADER]);
    PutWindowTilemap(sData->winIds[WIN_LEFT]);
    PutWindowTilemap(sData->winIds[WIN_RIGHT]);

    ShowBg(0);

    SetVBlankCallback(VBlankCB_CTB);
    SetGpuReg(REG_OFFSET_DISPCNT, DISPCNT_OBJ_ON | DISPCNT_OBJ_1D_MAP | DISPCNT_BG0_ON);
    SetMainCallback2(CB2_CTB_Main);

    // Build initial species and item lists
    CTB_PopulateSpecies();
    CTB_PopulateItems();

    CTB_DrawScreen();

    taskId = CreateTask(Task_CTB_Main, 0);
    (void)taskId;

    PlaySE(SE_SELECT);
}

// ---------------------------------------------------------------------------
// Main callback (runs each frame)
// ---------------------------------------------------------------------------
static void CB2_CTB_Main(void)
{
    AnimateSprites();
    BuildOamBuffer();
    UpdatePaletteFade();
    RunTasks();
}

// ---------------------------------------------------------------------------
// Populate species list (skip SPECIES_NONE, SPECIES_EGG, empty entries)
// ---------------------------------------------------------------------------
static void CTB_PopulateSpecies(void)
{
    u16 i;
    sData->speciesCount = 0;
    for (i = 1; i < NUM_SPECIES && sData->speciesCount < MAX_SPECIES_LIST; i++)
    {
        if (i == SPECIES_EGG)
            continue;
        if (gSpeciesInfo[i].baseHP == 0)
            continue;
        // Filter by letter
        if (sData->filterLetter > 0)
        {
            const u8 *name = GetSpeciesName(i);
            // GBA charmap: uppercase A=0xBB, so skip species whose first byte doesn't match
            // We store filterLetter as 0=all, 1-26 = A-Z offset into GBA charmap
            u8 gbaCh = 0xBB + (sData->filterLetter - 1);
            if (name[0] != gbaCh)
                continue;
        }
        sData->speciesList[sData->speciesCount++] = i;
    }
}

// ---------------------------------------------------------------------------
// Populate learnset moves for a species (level-up, level <= 50)
// ---------------------------------------------------------------------------
static void CTB_PopulateMoves(u16 species)
{
    u16 i = 0;
    bool8 alreadyHave;
    u16 m;
    const struct LevelUpMove *learnset = GetSpeciesLevelUpLearnset(species);
    sData->moveCount = 0;
    if (learnset == NULL)
        return;
    while (learnset[i].move != LEVEL_UP_MOVE_END && sData->moveCount < MAX_LEARNSET_MOVES)
    {
        m = learnset[i].move;
        if (learnset[i].level <= 50 && m != MOVE_NONE)
        {
            // Deduplicate
            alreadyHave = FALSE;
            {
                u16 j;
                for (j = 0; j < sData->moveCount; j++)
                {
                    if (sData->moveList[j] == m)
                    {
                        alreadyHave = TRUE;
                        break;
                    }
                }
            }
            if (!alreadyHave)
                sData->moveList[sData->moveCount++] = m;
        }
        i++;
    }
}

// ---------------------------------------------------------------------------
// Populate item list (all items; ITEM_NONE first)
// ---------------------------------------------------------------------------
static void CTB_PopulateItems(void)
{
    u16 i;
    sData->itemCount = 0;
    sData->itemList[sData->itemCount++] = ITEM_NONE;
    for (i = 1; i < ITEMS_COUNT && sData->itemCount < MAX_ITEM_LIST; i++)
    {
        // Filter by letter
        if (sData->filterLetter > 0)
        {
            u8 buf[32];
            CopyItemName(i, buf);
            u8 gbaCh = 0xBB + (sData->filterLetter - 1);
            if (buf[0] != gbaCh)
                continue;
        }
        sData->itemList[sData->itemCount++] = i;
    }
}

// ---------------------------------------------------------------------------
// Clamp cursor to list bounds
// ---------------------------------------------------------------------------
static void CTB_ClampCursor(void)
{
    u16 maxCount = 0;
    switch (sData->screen)
    {
    case SCREEN_TEAM_OVERVIEW:
        maxCount = sData->teamCount + 1; // slots + "add" option (+ maybe confirm)
        if (sData->teamCount > 0) maxCount++; // confirm button
        break;
    case SCREEN_SELECT_SPECIES: maxCount = sData->speciesCount; break;
    case SCREEN_SELECT_MOVES:   maxCount = sData->moveCount; break;
    case SCREEN_SELECT_NATURE:  maxCount = NUM_NATURES; break;
    case SCREEN_SELECT_ABILITY:
        {
            u16 sp = sData->team[sData->curTeamSlot].species;
            u8 abilCount = 0;
            u8 a;
            for (a = 0; a < NUM_ABILITY_SLOTS; a++)
                if (gSpeciesInfo[sp].abilities[a] != ABILITY_NONE)
                    abilCount++;
            maxCount = abilCount;
        }
        break;
    case SCREEN_SELECT_ITEM:   maxCount = sData->itemCount; break;
    case SCREEN_SELECT_TERA:   maxCount = NUMBER_OF_MON_TYPES - 2; break; // skip NONE and MYSTERY
    case SCREEN_SET_EVS:       maxCount = NUM_STATS; break;
    case SCREEN_SET_IVS:       maxCount = NUM_STATS; break;
    default: return;
    }
    if (maxCount == 0) maxCount = 1;
    if (sData->listCursor >= maxCount)
        sData->listCursor = maxCount - 1;
    // Adjust scroll (show 7 lines)
    if (sData->listCursor < sData->listScroll)
        sData->listScroll = sData->listCursor;
    if (sData->listCursor >= sData->listScroll + 7)
        sData->listScroll = sData->listCursor - 6;
}

// ---------------------------------------------------------------------------
// Utility: print a list line (highlighted or not)
// ---------------------------------------------------------------------------
#define LIST_LINE_H 16
static void CTB_ScrolledLine(u8 winId, u8 fontId, const u8 *str, u8 lineIdx, bool8 selected)
{
    u8 y = lineIdx * LIST_LINE_H;
    if (selected)
    {
        // Draw a highlight bar
        FillWindowPixelRect(winId, PIXEL_FILL(2), 0, y, 0x88, LIST_LINE_H);
        AddTextPrinterParameterized(winId, fontId, str, 4, y + 2, TEXT_SKIP_DRAW, NULL);
    }
    else
    {
        AddTextPrinterParameterized(winId, fontId, str, 4, y + 2, TEXT_SKIP_DRAW, NULL);
    }
}

// ---------------------------------------------------------------------------
// Mon icon management
// ---------------------------------------------------------------------------
static void CTB_ClearIcon(void)
{
    if (sData->iconSpriteId != ICON_SPRITE_NONE)
    {
        FreeAndDestroyMonIconSprite(&gSprites[sData->iconSpriteId]);
        sData->iconSpriteId = ICON_SPRITE_NONE;
    }
}

static void CTB_ShowMonIcon(u16 species)
{
    CTB_ClearIcon();
    if (species != SPECIES_NONE && species != SPECIES_EGG)
    {
        // Position icon in right panel centre (pixel 204, 64)
        sData->iconSpriteId = CreateMonIcon(species, SpriteCB_MonIcon, 204, 64, 0, species);
    }
}

static void CTB_ClearTeamIcons(void)
{
    u8 i;
    for (i = 0; i < MAX_TEAM_SIZE; i++)
    {
        if (sData->teamIconIds[i] != ICON_SPRITE_NONE)
        {
            FreeAndDestroyMonIconSprite(&gSprites[sData->teamIconIds[i]]);
            sData->teamIconIds[i] = ICON_SPRITE_NONE;
        }
    }
}

// ---------------------------------------------------------------------------
// Draw helper: clear both main windows
// ---------------------------------------------------------------------------
static void CTB_ClearWindows(void)
{
    FillWindowPixelBuffer(sData->winIds[WIN_LEFT],   PIXEL_FILL(1));
    FillWindowPixelBuffer(sData->winIds[WIN_RIGHT],  PIXEL_FILL(1));
    FillWindowPixelBuffer(sData->winIds[WIN_HEADER], PIXEL_FILL(1));
}

static void CTB_FlushWindows(void)
{
    CopyWindowToVram(sData->winIds[WIN_HEADER], COPYWIN_FULL);
    CopyWindowToVram(sData->winIds[WIN_LEFT],   COPYWIN_FULL);
    CopyWindowToVram(sData->winIds[WIN_RIGHT],  COPYWIN_FULL);
    ScheduleBgCopyTilemapToVram(0);
}

static void CTB_DrawHeader(const u8 *title)
{
    AddTextPrinterParameterized(sData->winIds[WIN_HEADER], FONT_NORMAL, title, 4, 4, TEXT_SKIP_DRAW, NULL);
}

// ---------------------------------------------------------------------------
// SCREEN: TEAM OVERVIEW
// ---------------------------------------------------------------------------
static void CTB_DrawOverview(void)
{
    u8 i;
    u8 lineIdx = 0;
    u8 buf[32];

    CTB_DrawHeader(sText_Header_Overview);

    // Team slots
    for (i = 0; i < sData->teamCount; i++)
    {
        const u8 *name = GetSpeciesName(sData->team[i].species);
        StringCopy(buf, name);
        CTB_ScrolledLine(sData->winIds[WIN_LEFT], FONT_NORMAL, buf, lineIdx, (sData->listCursor == i));
        lineIdx++;
    }

    // "Add Pokemon" entry (if room)
    if (sData->teamCount < MAX_TEAM_SIZE)
    {
        CTB_ScrolledLine(sData->winIds[WIN_LEFT], FONT_SMALL, sText_AddPokemon, lineIdx,
            (sData->listCursor == sData->teamCount));
        lineIdx++;
    }

    // Confirm entry (if at least 1 mon)
    if (sData->teamCount > 0)
    {
        CTB_ScrolledLine(sData->winIds[WIN_LEFT], FONT_SMALL, sText_Confirm, lineIdx,
            (sData->listCursor == sData->teamCount + (sData->teamCount < MAX_TEAM_SIZE ? 1 : 0)));
    }

    AddTextPrinterParameterized(sData->winIds[WIN_LEFT], FONT_SMALL, sText_PressStart,
        4, 14 * LIST_LINE_H, TEXT_SKIP_DRAW, NULL);

    // Right panel: preview selected mon
    if (sData->listCursor < sData->teamCount)
    {
        const struct CustomBuilderMon *cm = &sData->team[sData->listCursor];
        u8 y = 0;

        CTB_ShowMonIcon(cm->species);

        // Show nature
        AddTextPrinterParameterized(sData->winIds[WIN_RIGHT], FONT_SMALL,
            gNaturesInfo[cm->nature].name, 4, y + 64, TEXT_SKIP_DRAW, NULL);
        y += LIST_LINE_H;

        // Show ability
        {
            enum Ability ab = gSpeciesInfo[cm->species].abilities[cm->abilityNum];
            if (ab != ABILITY_NONE)
                AddTextPrinterParameterized(sData->winIds[WIN_RIGHT], FONT_SMALL,
                    gAbilitiesInfo[ab].name, 4, 64 + y, TEXT_SKIP_DRAW, NULL);
        }
        y += LIST_LINE_H;

        // Show item
        if (cm->item != ITEM_NONE)
        {
            CopyItemName(cm->item, buf);
            AddTextPrinterParameterized(sData->winIds[WIN_RIGHT], FONT_SMALL, buf, 4, 64 + y, TEXT_SKIP_DRAW, NULL);
        }
        y += LIST_LINE_H;

        // Moves
        {
            u8 m;
            for (m = 0; m < MAX_MON_MOVES; m++)
            {
                const u8 *moveName = (cm->moves[m] != MOVE_NONE)
                    ? GetMoveName(cm->moves[m])
                    : sText_Empty;
                AddTextPrinterParameterized(sData->winIds[WIN_RIGHT], FONT_SMALL,
                    moveName, 4, 64 + y, TEXT_SKIP_DRAW, NULL);
                if (m < MAX_MON_MOVES - 1) y += LIST_LINE_H;
            }
        }
    }
    else
    {
        CTB_ClearIcon();
    }

    // Team icons along top of right panel
    {
        u8 iconX = 140; // approx pixel X start for right panel
        for (i = 0; i < MAX_TEAM_SIZE; i++)
        {
            if (sData->teamIconIds[i] != ICON_SPRITE_NONE)
            {
                FreeAndDestroyMonIconSprite(&gSprites[sData->teamIconIds[i]]);
                sData->teamIconIds[i] = ICON_SPRITE_NONE;
            }
            if (i < sData->teamCount)
            {
                sData->teamIconIds[i] = CreateMonIcon(sData->team[i].species, SpriteCB_MonIcon,
                    iconX + (i % 3) * 36, 8 + (i / 3) * 28, 1, sData->team[i].species);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// SCREEN: SELECT SPECIES
// ---------------------------------------------------------------------------
static void CTB_DrawSpeciesList(void)
{
    u8 i;
    u8 buf[32];

    CTB_DrawHeader(sText_Header_Species);
    CTB_ClampCursor();

    // Filter letter display
    {
        StringCopy(buf, sText_Filter);
        if (sData->filterLetter == 0)
            StringAppend(buf, sText_Empty);
        else
        {
            buf[StringLength(buf)] = 0xBB + (sData->filterLetter - 1);
            buf[StringLength(buf)] = EOS;
        }
        AddTextPrinterParameterized(sData->winIds[WIN_LEFT], FONT_SMALL, buf, 4, 0, TEXT_SKIP_DRAW, NULL);
    }

    // List
    for (i = 0; i < 7 && (sData->listScroll + i) < sData->speciesCount; i++)
    {
        u16 specIdx = sData->listScroll + i;
        const u8 *name = GetSpeciesName(sData->speciesList[specIdx]);
        CTB_ScrolledLine(sData->winIds[WIN_LEFT], FONT_NORMAL, name, i + 1, (sData->listCursor == specIdx));
    }

    // Key help
    AddTextPrinterParameterized(sData->winIds[WIN_LEFT], FONT_SMALL, sText_PressA,
        4, 14 * LIST_LINE_H, TEXT_SKIP_DRAW, NULL);

    // Right panel: show selected species info
    if (sData->speciesCount > 0)
    {
        u16 sp = sData->speciesList[sData->listCursor];
        CTB_ShowMonIcon(sp);

        // Types
        {
            const struct TypeInfo *t1 = &gTypesInfo[gSpeciesInfo[sp].types[0]];
            const struct TypeInfo *t2 = &gTypesInfo[gSpeciesInfo[sp].types[1]];
            StringCopy(buf, t1->name);
            if (gSpeciesInfo[sp].types[1] != gSpeciesInfo[sp].types[0])
            {
                StringAppend(buf, sText_Slash);
                StringAppend(buf, t2->name);
            }
            AddTextPrinterParameterized(sData->winIds[WIN_RIGHT], FONT_SMALL, buf, 4, 64, TEXT_SKIP_DRAW, NULL);
        }

        // Base stats compact
        {
            u8 y = 64 + LIST_LINE_H;
            const u8 *statNames[] = {sText_Stat_HP, sText_Stat_Atk, sText_Stat_Def,
                                     sText_Stat_SpA, sText_Stat_SpD, sText_Stat_Spe};
            const u8 baseStats[NUM_STATS] = {
                gSpeciesInfo[sp].baseHP, gSpeciesInfo[sp].baseAttack,
                gSpeciesInfo[sp].baseDefense, gSpeciesInfo[sp].baseSpAttack,
                gSpeciesInfo[sp].baseSpDefense, gSpeciesInfo[sp].baseSpeed
            };
            u8 s;
            for (s = 0; s < NUM_STATS; s++)
            {
                StringCopy(buf, statNames[s]);
                StringAppend(buf, sText_Colon);
                ConvertIntToDecimalStringN(numStr, baseStats[s], STR_CONV_MODE_LEFT_ALIGN, 3);
                StringAppend(buf, numStr);
                AddTextPrinterParameterized(sData->winIds[WIN_RIGHT], FONT_SMALL, buf, 4, y, TEXT_SKIP_DRAW, NULL);
                y += LIST_LINE_H;
            }
        }
    }
}

// ---------------------------------------------------------------------------
// SCREEN: SELECT MOVES
// ---------------------------------------------------------------------------
static void CTB_DrawMoveList(void)
{
    u8 i;
    u8 buf[48];
    u8 numStr[8];
    struct CustomBuilderMon *cm = &sData->team[sData->curTeamSlot];

    CTB_DrawHeader(sText_Header_Moves);
    CTB_ClampCursor();

    // Current move slots (top of left panel)
    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        const u8 *moveName = (cm->moves[i] != MOVE_NONE) ? GetMoveName(cm->moves[i]) : sText_Empty;
        u8 arrow = (i == sData->curMoveSlot) ? 0xAE : 0x20; // right arrow or space in charmap
        buf[0] = arrow;
        buf[1] = EOS;
        StringAppend(buf, moveName);
        AddTextPrinterParameterized(sData->winIds[WIN_LEFT], FONT_SMALL, buf, 4, i * LIST_LINE_H, TEXT_SKIP_DRAW, NULL);
    }

    // Separator line
    AddTextPrinterParameterized(sData->winIds[WIN_LEFT], FONT_SMALL,
        _("-- Scegli mossa --"), 4, 4 * LIST_LINE_H + 4, TEXT_SKIP_DRAW, NULL);

    // Move list
    for (i = 0; i < 7 && (sData->listScroll + i) < sData->moveCount; i++)
    {
        u16 moveIdx = sData->listScroll + i;
        const u8 *moveName = GetMoveName(sData->moveList[moveIdx]);
        CTB_ScrolledLine(sData->winIds[WIN_LEFT], FONT_NORMAL, moveName, i + 5 + 1, (sData->listCursor == moveIdx));
    }

    AddTextPrinterParameterized(sData->winIds[WIN_LEFT], FONT_SMALL, sText_PressA,
        4, 14 * LIST_LINE_H, TEXT_SKIP_DRAW, NULL);

    // Right panel: info about highlighted move
    if (sData->moveCount > 0)
    {
        u16 moveId = sData->moveList[sData->listCursor];
        const u8 *moveName = GetMoveName(moveId);
        u8 y = 0;

        AddTextPrinterParameterized(sData->winIds[WIN_RIGHT], FONT_NORMAL, moveName, 4, y, TEXT_SKIP_DRAW, NULL);
        y += LIST_LINE_H;

        // Type
        {
            enum Type t = GetMoveType(moveId);
            AddTextPrinterParameterized(sData->winIds[WIN_RIGHT], FONT_SMALL, gTypesInfo[t].name, 4, y, TEXT_SKIP_DRAW, NULL);
            y += LIST_LINE_H;
        }

        // Power
        {
            u32 power = GetMovePower(moveId);
            StringCopy(buf, _("Pot:"));
            if (power == 0)
                StringAppend(buf, _("---"));
            else
            {
                ConvertIntToDecimalStringN(numStr, power, STR_CONV_MODE_LEFT_ALIGN, 3);
                StringAppend(buf, numStr);
            }
            AddTextPrinterParameterized(sData->winIds[WIN_RIGHT], FONT_SMALL, buf, 4, y, TEXT_SKIP_DRAW, NULL);
            y += LIST_LINE_H;
        }

        // PP
        {
            StringCopy(buf, _("PP:"));
            ConvertIntToDecimalStringN(numStr, GetMovePP(moveId), STR_CONV_MODE_LEFT_ALIGN, 2);
            StringAppend(buf, numStr);
            AddTextPrinterParameterized(sData->winIds[WIN_RIGHT], FONT_SMALL, buf, 4, y, TEXT_SKIP_DRAW, NULL);
        }

        CTB_ShowMonIcon(cm->species);
    }
}

// ---------------------------------------------------------------------------
// SCREEN: SELECT NATURE
// ---------------------------------------------------------------------------
static void CTB_DrawNatureList(void)
{
    u8 i;
    u8 buf[32];

    CTB_DrawHeader(sText_Header_Nature);
    CTB_ClampCursor();

    for (i = 0; i < 8 && (sData->listScroll + i) < NUM_NATURES; i++)
    {
        u8 nat = sData->listScroll + i;
        const u8 *natName = gNaturesInfo[nat].name;
        CTB_ScrolledLine(sData->winIds[WIN_LEFT], FONT_NORMAL, natName, i, (sData->listCursor == nat));
    }

    AddTextPrinterParameterized(sData->winIds[WIN_LEFT], FONT_SMALL, sText_PressA,
        4, 14 * LIST_LINE_H, TEXT_SKIP_DRAW, NULL);

    // Right panel: stat modifiers for highlighted nature
    {
        u8 nat = (u8)sData->listCursor;
        enum Stat statUp   = gNaturesInfo[nat].statUp;
        enum Stat statDown = gNaturesInfo[nat].statDown;
        u8 y = 0;
        u8 s;
        const u8 *arrows[NUM_STATS];

        AddTextPrinterParameterized(sData->winIds[WIN_RIGHT], FONT_NORMAL,
            gNaturesInfo[nat].name, 4, y, TEXT_SKIP_DRAW, NULL);
        y += LIST_LINE_H + 4;

        for (s = 0; s < NUM_STATS; s++)
        {
            const u8 *arrow;
            if (statUp == statDown)
                arrow = sText_Neutral;
            else if (statUp != STAT_HP && s == statUp - 1) // statUp 1=ATK..
                arrow = sText_Up;
            else if (statDown != STAT_HP && s == statDown - 1)
                arrow = sText_Down;
            else
                arrow = sText_Neutral;

            StringCopy(buf, sStatNames[s]);
            StringAppend(buf, sText_Space);
            StringAppend(buf, arrow);
            AddTextPrinterParameterized(sData->winIds[WIN_RIGHT], FONT_SMALL, buf, 4, y, TEXT_SKIP_DRAW, NULL);
            y += LIST_LINE_H;
        }

        CTB_ShowMonIcon(sData->team[sData->curTeamSlot].species);
    }
}

// ---------------------------------------------------------------------------
// SCREEN: SELECT ABILITY
// ---------------------------------------------------------------------------
static void CTB_DrawAbilityList(void)
{
    u8 i;
    u8 buf[32];
    u16 sp = sData->team[sData->curTeamSlot].species;

    CTB_DrawHeader(sText_Header_Ability);
    CTB_ClampCursor();

    {
        u8 lineIdx = 0;
        for (i = 0; i < NUM_ABILITY_SLOTS; i++)
        {
            enum Ability ab = gSpeciesInfo[sp].abilities[i];
            if (ab == ABILITY_NONE)
                continue;

            StringCopy(buf, gAbilitiesInfo[ab].name);
            if (i == 2) // hidden ability
            {
                StringAppend(buf, sText_Space);
                StringAppend(buf, sText_HiddenAbil);
            }
            CTB_ScrolledLine(sData->winIds[WIN_LEFT], FONT_NORMAL, buf, lineIdx,
                (sData->listCursor == lineIdx));
            lineIdx++;
        }
    }

    AddTextPrinterParameterized(sData->winIds[WIN_LEFT], FONT_SMALL, sText_PressA,
        4, 14 * LIST_LINE_H, TEXT_SKIP_DRAW, NULL);

    // Right: ability description
    {
        u16 abilIdx = 0;
        u8 lineIdx = 0;
        for (i = 0; i < NUM_ABILITY_SLOTS; i++)
        {
            enum Ability ab = gSpeciesInfo[sp].abilities[i];
            if (ab == ABILITY_NONE) continue;
            if (lineIdx == sData->listCursor)
            {
                abilIdx = (u16)ab;
                break;
            }
            lineIdx++;
        }
        AddTextPrinterParameterized(sData->winIds[WIN_RIGHT], FONT_NORMAL,
            gAbilitiesInfo[abilIdx].name, 4, 0, TEXT_SKIP_DRAW, NULL);
        if (gAbilitiesInfo[abilIdx].description != NULL)
            AddTextPrinterParameterized(sData->winIds[WIN_RIGHT], FONT_SMALL,
                gAbilitiesInfo[abilIdx].description, 4, LIST_LINE_H + 4, TEXT_SKIP_DRAW, NULL);
        CTB_ShowMonIcon(sp);
    }
}

// ---------------------------------------------------------------------------
// SCREEN: SELECT ITEM
// ---------------------------------------------------------------------------
static void CTB_DrawItemList(void)
{
    u8 i;
    u8 buf[32];

    CTB_DrawHeader(sText_Header_Item);
    CTB_ClampCursor();

    // Filter display
    {
        StringCopy(buf, sText_Filter);
        if (sData->filterLetter == 0)
            StringAppend(buf, sText_Empty);
        else
        {
            buf[StringLength(buf)] = 0xBB + (sData->filterLetter - 1);
            buf[StringLength(buf)] = EOS;
        }
        AddTextPrinterParameterized(sData->winIds[WIN_LEFT], FONT_SMALL, buf, 4, 0, TEXT_SKIP_DRAW, NULL);
    }

    for (i = 0; i < 7 && (sData->listScroll + i) < sData->itemCount; i++)
    {
        u16 itemIdx = sData->listScroll + i;
        u16 itemId  = sData->itemList[itemIdx];
        if (itemId == ITEM_NONE)
            StringCopy(buf, sText_Nessuno);
        else
            CopyItemName(itemId, buf);
        CTB_ScrolledLine(sData->winIds[WIN_LEFT], FONT_NORMAL, buf, i + 1, (sData->listCursor == itemIdx));
    }

    AddTextPrinterParameterized(sData->winIds[WIN_LEFT], FONT_SMALL, sText_PressA,
        4, 14 * LIST_LINE_H, TEXT_SKIP_DRAW, NULL);

    // Right: selected item name
    if (sData->itemCount > 0)
    {
        u16 itemId = sData->itemList[sData->listCursor];
        if (itemId == ITEM_NONE)
            AddTextPrinterParameterized(sData->winIds[WIN_RIGHT], FONT_NORMAL, sText_Nessuno, 4, 0, TEXT_SKIP_DRAW, NULL);
        else
        {
            CopyItemName(itemId, buf);
            AddTextPrinterParameterized(sData->winIds[WIN_RIGHT], FONT_NORMAL, buf, 4, 0, TEXT_SKIP_DRAW, NULL);
        }
        CTB_ShowMonIcon(sData->team[sData->curTeamSlot].species);
    }
}

// ---------------------------------------------------------------------------
// SCREEN: SELECT TERA TYPE
// (skip TYPE_NONE=0 and TYPE_MYSTERY=10)
// ---------------------------------------------------------------------------
static const u8 sTeraTypes[] = {
    TYPE_NORMAL, TYPE_FIGHTING, TYPE_FLYING, TYPE_POISON, TYPE_GROUND,
    TYPE_ROCK, TYPE_BUG, TYPE_GHOST, TYPE_STEEL, TYPE_FIRE, TYPE_WATER,
    TYPE_GRASS, TYPE_ELECTRIC, TYPE_PSYCHIC, TYPE_ICE, TYPE_DRAGON,
    TYPE_DARK, TYPE_FAIRY
};
#define NUM_TERA_TYPES ARRAY_COUNT(sTeraTypes)

static void CTB_DrawTeraList(void)
{
    u8 i;

    CTB_DrawHeader(sText_Header_Tera);
    CTB_ClampCursor();

    for (i = 0; i < 8 && (sData->listScroll + i) < NUM_TERA_TYPES; i++)
    {
        u8 typeIdx = sData->listScroll + i;
        CTB_ScrolledLine(sData->winIds[WIN_LEFT], FONT_NORMAL,
            gTypesInfo[sTeraTypes[typeIdx]].name, i, (sData->listCursor == typeIdx));
    }

    AddTextPrinterParameterized(sData->winIds[WIN_LEFT], FONT_SMALL, sText_PressA,
        4, 14 * LIST_LINE_H, TEXT_SKIP_DRAW, NULL);

    if (sData->listCursor < NUM_TERA_TYPES)
    {
        AddTextPrinterParameterized(sData->winIds[WIN_RIGHT], FONT_NORMAL,
            gTypesInfo[sTeraTypes[sData->listCursor]].name, 4, 0, TEXT_SKIP_DRAW, NULL);
        CTB_ShowMonIcon(sData->team[sData->curTeamSlot].species);
    }
}

// ---------------------------------------------------------------------------
// SCREEN: SET EVs / IVs
// ---------------------------------------------------------------------------
static void CTB_DrawStatScreen(bool8 isEV)
{
    u8 i;
    u8 buf[32];
    u8 numStr[8];
    struct CustomBuilderMon *cm = &sData->team[sData->curTeamSlot];

    CTB_DrawHeader(isEV ? sText_Header_EVs : sText_Header_IVs);

    u16 totalEV = 0;
    if (isEV)
        for (i = 0; i < NUM_STATS; i++) totalEV += cm->evs[i];

    for (i = 0; i < NUM_STATS; i++)
    {
        u16 val = isEV ? cm->evs[i] : cm->ivs[i];
        StringCopy(buf, sStatNames[i]);
        StringAppend(buf, sText_Colon);
        ConvertIntToDecimalStringN(numStr, val, STR_CONV_MODE_LEFT_ALIGN, 3);
        StringAppend(buf, numStr);
        CTB_ScrolledLine(sData->winIds[WIN_LEFT], FONT_NORMAL, buf, i, (sData->curStatIdx == i));
    }

    // Total EV display
    if (isEV)
    {
        StringCopy(buf, sText_TotalEV);
        ConvertIntToDecimalStringN(numStr, totalEV, STR_CONV_MODE_LEFT_ALIGN, 3);
        StringAppend(buf, numStr);
        StringAppend(buf, _("/510"));
        AddTextPrinterParameterized(sData->winIds[WIN_LEFT], FONT_SMALL, buf, 4, 7 * LIST_LINE_H, TEXT_SKIP_DRAW, NULL);
    }

    AddTextPrinterParameterized(sData->winIds[WIN_LEFT], FONT_SMALL, sText_LR,
        4, 14 * LIST_LINE_H, TEXT_SKIP_DRAW, NULL);

    // Right: numeric display of highlighted stat
    {
        u16 val = isEV ? cm->evs[sData->curStatIdx] : cm->ivs[sData->curStatIdx];
        AddTextPrinterParameterized(sData->winIds[WIN_RIGHT], FONT_NORMAL,
            sStatNames[sData->curStatIdx], 4, 0, TEXT_SKIP_DRAW, NULL);
        ConvertIntToDecimalStringN(numStr, val, STR_CONV_MODE_LEFT_ALIGN, 3);
        AddTextPrinterParameterized(sData->winIds[WIN_RIGHT], FONT_NORMAL, numStr, 4, LIST_LINE_H + 4, TEXT_SKIP_DRAW, NULL);
        CTB_ShowMonIcon(cm->species);
    }
}

static void CTB_DrawEVsScreen(void) { CTB_DrawStatScreen(TRUE);  }
static void CTB_DrawIVsScreen(void) { CTB_DrawStatScreen(FALSE); }

// ---------------------------------------------------------------------------
// Main draw dispatcher
// ---------------------------------------------------------------------------
static void CTB_DrawScreen(void)
{
    CTB_ClearWindows();
    switch (sData->screen)
    {
    case SCREEN_TEAM_OVERVIEW:  CTB_DrawOverview();     break;
    case SCREEN_SELECT_SPECIES: CTB_DrawSpeciesList();  break;
    case SCREEN_SELECT_MOVES:   CTB_DrawMoveList();     break;
    case SCREEN_SELECT_NATURE:  CTB_DrawNatureList();   break;
    case SCREEN_SELECT_ABILITY: CTB_DrawAbilityList();  break;
    case SCREEN_SELECT_ITEM:    CTB_DrawItemList();     break;
    case SCREEN_SELECT_TERA:    CTB_DrawTeraList();     break;
    case SCREEN_SET_EVS:        CTB_DrawEVsScreen();    break;
    case SCREEN_SET_IVS:        CTB_DrawIVsScreen();    break;
    default: break;
    }
    CTB_FlushWindows();
}

// ---------------------------------------------------------------------------
// Build team and exit back to field
// ---------------------------------------------------------------------------
static void CTB_BuildAndExitTeam(u8 taskId)
{
    u8 i;

    ZeroPlayerPartyMons();
    gPartiesCount[B_TRAINER_PLAYER] = sData->teamCount;

    for (i = 0; i < sData->teamCount; i++)
    {
        struct CustomBuilderMon *cm = &sData->team[i];
        struct PokemonTemplate tmpl = {0};

        tmpl.species             = cm->species;
        tmpl.level               = 50;
        tmpl.heldItem            = cm->item;
        tmpl.ball                = ITEM_POKE_BALL;
        tmpl.nature              = cm->nature;
        tmpl.gender              = MON_GENDER_RANDOM;
        tmpl.origin              = GIFTMON_ORIGIN;
        tmpl.abilityNum          = cm->abilityNum;
        tmpl.teraType            = cm->teraType;
        tmpl.isShiny             = FALSE;
        tmpl.doNotUseDefaultShinyness  = TRUE;
        tmpl.doNotUseDefaultAbility    = TRUE;
        tmpl.doNotUseDefaultTeraType   = (cm->teraType != TYPE_NONE);
        tmpl.ignoreTotalEvCheck        = TRUE;

        {
            u8 s;
            for (s = 0; s < NUM_STATS; s++)
            {
                tmpl.evs[s] = cm->evs[s];
                tmpl.ivs[s] = cm->ivs[s];
            }
            for (s = 0; s < MAX_MON_MOVES; s++)
                tmpl.moves[s] = cm->moves[s];
        }

        CreateMonFromTemplate(&gParties[B_TRAINER_PLAYER][i], &tmpl);
    }

    gSpecialVar_Result = sData->teamCount;

    CTB_ClearIcon();
    CTB_ClearTeamIcons();
    FreeMonIconPalettes();
    FreeAllWindowBuffers();
    Free(sData);
    sData = NULL;

    DestroyTask(taskId);
    ScriptContext_Enable();
    SetMainCallback2(CB2_ReturnToField);
}

// ---------------------------------------------------------------------------
// Input handler transitions
// ---------------------------------------------------------------------------
static void CTB_EnterScreen(u8 screen)
{
    sData->screen     = screen;
    sData->listCursor = 0;
    sData->listScroll = 0;
    CTB_DrawScreen();
}

static void CTB_StartEditing(u8 slot)
{
    sData->curTeamSlot = slot;
    CTB_PopulateMoves(sData->team[slot].species);
    sData->filterLetter = 0;
    CTB_EnterScreen(SCREEN_SELECT_MOVES);
}

static void CTB_StartAddingPokemon(void)
{
    u8 slot = sData->teamCount;
    // Initialize with defaults
    sData->team[slot].species    = SPECIES_NONE;
    sData->team[slot].nature     = NATURE_HARDY;
    sData->team[slot].abilityNum = 0;
    sData->team[slot].item       = ITEM_NONE;
    sData->team[slot].teraType   = TYPE_NONE;
    {
        u8 s;
        for (s = 0; s < NUM_STATS; s++) { sData->team[slot].evs[s] = 0; sData->team[slot].ivs[s] = 31; }
        for (s = 0; s < MAX_MON_MOVES; s++) sData->team[slot].moves[s] = MOVE_NONE;
    }
    sData->curTeamSlot = slot;
    sData->filterLetter = 0;
    CTB_PopulateSpecies();
    CTB_EnterScreen(SCREEN_SELECT_SPECIES);
}

// ---------------------------------------------------------------------------
// Main task
// ---------------------------------------------------------------------------
static void Task_CTB_Main(u8 taskId)
{
    if (gPaletteFade.active)
        return;

    // --- Navigation ---
    if (JOY_NEW(DPAD_UP))
    {
        if (sData->screen == SCREEN_SET_EVS || sData->screen == SCREEN_SET_IVS)
        {
            if (sData->curStatIdx > 0) sData->curStatIdx--;
        }
        else
        {
            if (sData->listCursor > 0) sData->listCursor--;
            else
            {
                // Wrap to end of list
                switch (sData->screen)
                {
                case SCREEN_SELECT_SPECIES: sData->listCursor = (sData->speciesCount > 0) ? sData->speciesCount - 1 : 0; break;
                case SCREEN_SELECT_MOVES:   sData->listCursor = (sData->moveCount > 0) ? sData->moveCount - 1 : 0; break;
                case SCREEN_SELECT_NATURE:  sData->listCursor = NUM_NATURES - 1; break;
                case SCREEN_SELECT_ABILITY:
                {
                    u8 cnt = 0;
                    u8 a;
                    for (a = 0; a < NUM_ABILITY_SLOTS; a++)
                        if (gSpeciesInfo[sData->team[sData->curTeamSlot].species].abilities[a] != ABILITY_NONE)
                            cnt++;
                    sData->listCursor = (cnt > 0) ? cnt - 1 : 0;
                    break;
                }
                case SCREEN_SELECT_ITEM:    sData->listCursor = (sData->itemCount > 0) ? sData->itemCount - 1 : 0; break;
                case SCREEN_SELECT_TERA:    sData->listCursor = NUM_TERA_TYPES - 1; break;
                default: break;
                }
            }
        }
        CTB_ClampCursor();
        CTB_DrawScreen();
        PlaySE(SE_SELECT);
    }
    else if (JOY_NEW(DPAD_DOWN))
    {
        if (sData->screen == SCREEN_SET_EVS || sData->screen == SCREEN_SET_IVS)
        {
            if (sData->curStatIdx < NUM_STATS - 1) sData->curStatIdx++;
        }
        else
        {
            u16 maxIdx = 0;
            switch (sData->screen)
            {
            case SCREEN_TEAM_OVERVIEW:
            {
                u16 total = sData->teamCount + (sData->teamCount < MAX_TEAM_SIZE ? 1 : 0)
                            + (sData->teamCount > 0 ? 1 : 0);
                maxIdx = total > 0 ? total - 1 : 0;
                break;
            }
            case SCREEN_SELECT_SPECIES: maxIdx = sData->speciesCount > 0 ? sData->speciesCount - 1 : 0; break;
            case SCREEN_SELECT_MOVES:   maxIdx = sData->moveCount > 0 ? sData->moveCount - 1 : 0; break;
            case SCREEN_SELECT_NATURE:  maxIdx = NUM_NATURES - 1; break;
            case SCREEN_SELECT_ABILITY:
            {
                u8 cnt = 0;
                u8 a;
                for (a = 0; a < NUM_ABILITY_SLOTS; a++)
                    if (gSpeciesInfo[sData->team[sData->curTeamSlot].species].abilities[a] != ABILITY_NONE)
                        cnt++;
                maxIdx = cnt > 0 ? cnt - 1 : 0;
                break;
            }
            case SCREEN_SELECT_ITEM:   maxIdx = sData->itemCount > 0 ? sData->itemCount - 1 : 0; break;
            case SCREEN_SELECT_TERA:   maxIdx = NUM_TERA_TYPES - 1; break;
            default: break;
            }
            if (sData->listCursor < maxIdx)
                sData->listCursor++;
            else
                sData->listCursor = 0; // wrap
        }
        CTB_ClampCursor();
        CTB_DrawScreen();
        PlaySE(SE_SELECT);
    }
    // --- L/R: cycle filter letter (species & item screens) or change EV/IV value ---
    else if (JOY_NEW(L_BUTTON))
    {
        if (sData->screen == SCREEN_SET_EVS)
        {
            struct CustomBuilderMon *cm = &sData->team[sData->curTeamSlot];
            if (cm->evs[sData->curStatIdx] >= 4)
                cm->evs[sData->curStatIdx] -= 4;
            else
                cm->evs[sData->curStatIdx] = 0;
            CTB_DrawScreen();
        }
        else if (sData->screen == SCREEN_SET_IVS)
        {
            struct CustomBuilderMon *cm = &sData->team[sData->curTeamSlot];
            if (cm->ivs[sData->curStatIdx] > 0)
                cm->ivs[sData->curStatIdx]--;
            CTB_DrawScreen();
        }
        else if (sData->screen == SCREEN_SELECT_SPECIES || sData->screen == SCREEN_SELECT_ITEM)
        {
            if (sData->filterLetter > 0) sData->filterLetter--;
            sData->listCursor = 0; sData->listScroll = 0;
            if (sData->screen == SCREEN_SELECT_SPECIES) CTB_PopulateSpecies();
            else CTB_PopulateItems();
            CTB_DrawScreen();
        }
        PlaySE(SE_SELECT);
    }
    else if (JOY_NEW(R_BUTTON))
    {
        if (sData->screen == SCREEN_SET_EVS)
        {
            struct CustomBuilderMon *cm = &sData->team[sData->curTeamSlot];
            u16 total = 0;
            u8 s;
            for (s = 0; s < NUM_STATS; s++) total += cm->evs[s];
            if (total + 4 <= EV_MAX_TOTAL && cm->evs[sData->curStatIdx] + 4 <= EV_MAX_PER_STAT)
                cm->evs[sData->curStatIdx] += 4;
            CTB_DrawScreen();
        }
        else if (sData->screen == SCREEN_SET_IVS)
        {
            struct CustomBuilderMon *cm = &sData->team[sData->curTeamSlot];
            if (cm->ivs[sData->curStatIdx] < 31)
                cm->ivs[sData->curStatIdx]++;
            CTB_DrawScreen();
        }
        else if (sData->screen == SCREEN_SELECT_SPECIES || sData->screen == SCREEN_SELECT_ITEM)
        {
            if (sData->filterLetter < 26) sData->filterLetter++;
            sData->listCursor = 0; sData->listScroll = 0;
            if (sData->screen == SCREEN_SELECT_SPECIES) CTB_PopulateSpecies();
            else CTB_PopulateItems();
            CTB_DrawScreen();
        }
        PlaySE(SE_SELECT);
    }
    // --- DPAD_LEFT on move screen: switch move slot ---
    else if (JOY_NEW(DPAD_LEFT) && sData->screen == SCREEN_SELECT_MOVES)
    {
        if (sData->curMoveSlot > 0) sData->curMoveSlot--;
        CTB_DrawScreen();
        PlaySE(SE_SELECT);
    }
    else if (JOY_NEW(DPAD_RIGHT) && sData->screen == SCREEN_SELECT_MOVES)
    {
        if (sData->curMoveSlot < MAX_MON_MOVES - 1) sData->curMoveSlot++;
        CTB_DrawScreen();
        PlaySE(SE_SELECT);
    }
    // --- A button: confirm selection ---
    else if (JOY_NEW(A_BUTTON))
    {
        switch (sData->screen)
        {
        case SCREEN_TEAM_OVERVIEW:
        {
            u8 addIdx = sData->teamCount;
            u8 confirmIdx = sData->teamCount + (sData->teamCount < MAX_TEAM_SIZE ? 1 : 0);

            if (sData->listCursor < sData->teamCount)
            {
                // Edit existing slot
                CTB_StartEditing(sData->listCursor);
            }
            else if (sData->teamCount < MAX_TEAM_SIZE && sData->listCursor == addIdx)
            {
                CTB_StartAddingPokemon();
            }
            else if (sData->teamCount > 0 && sData->listCursor == confirmIdx)
            {
                // Confirm and build
                CTB_BuildAndExitTeam(taskId);
                return;
            }
            PlaySE(SE_SELECT);
            break;
        }

        case SCREEN_SELECT_SPECIES:
            if (sData->speciesCount > 0)
            {
                sData->team[sData->curTeamSlot].species = sData->speciesList[sData->listCursor];
                // After species, go to move selection
                CTB_PopulateMoves(sData->team[sData->curTeamSlot].species);
                sData->curMoveSlot = 0;
                CTB_EnterScreen(SCREEN_SELECT_MOVES);
                PlaySE(SE_SELECT);
            }
            break;

        case SCREEN_SELECT_MOVES:
            if (sData->moveCount > 0)
            {
                sData->team[sData->curTeamSlot].moves[sData->curMoveSlot] =
                    sData->moveList[sData->listCursor];
                // Advance to next empty move slot or next screen
                {
                    u8 next = sData->curMoveSlot + 1;
                    if (next < MAX_MON_MOVES)
                        sData->curMoveSlot = next;
                }
                CTB_DrawScreen();
                PlaySE(SE_SELECT);
            }
            break;

        case SCREEN_SELECT_NATURE:
            sData->team[sData->curTeamSlot].nature = (u8)sData->listCursor;
            CTB_EnterScreen(SCREEN_SELECT_ABILITY);
            PlaySE(SE_SELECT);
            break;

        case SCREEN_SELECT_ABILITY:
        {
            u16 sp = sData->team[sData->curTeamSlot].species;
            u8 lineIdx = 0;
            u8 a;
            for (a = 0; a < NUM_ABILITY_SLOTS; a++)
            {
                if (gSpeciesInfo[sp].abilities[a] == ABILITY_NONE) continue;
                if (lineIdx == sData->listCursor)
                {
                    sData->team[sData->curTeamSlot].abilityNum = a;
                    break;
                }
                lineIdx++;
            }
            sData->filterLetter = 0;
            CTB_PopulateItems();
            CTB_EnterScreen(SCREEN_SELECT_ITEM);
            PlaySE(SE_SELECT);
            break;
        }

        case SCREEN_SELECT_ITEM:
            if (sData->itemCount > 0)
            {
                sData->team[sData->curTeamSlot].item = sData->itemList[sData->listCursor];
                CTB_EnterScreen(SCREEN_SELECT_TERA);
                PlaySE(SE_SELECT);
            }
            break;

        case SCREEN_SELECT_TERA:
            if (sData->listCursor < NUM_TERA_TYPES)
            {
                sData->team[sData->curTeamSlot].teraType = sTeraTypes[sData->listCursor];
                sData->curStatIdx = 0;
                CTB_EnterScreen(SCREEN_SET_EVS);
                PlaySE(SE_SELECT);
            }
            break;

        case SCREEN_SET_EVS:
            // A confirms EVs, goes to IVs
            sData->curStatIdx = 0;
            CTB_EnterScreen(SCREEN_SET_IVS);
            PlaySE(SE_SELECT);
            break;

        case SCREEN_SET_IVS:
            // A confirms IVs; mon is done — add to team (or update) and go back to overview
            if (sData->curTeamSlot == sData->teamCount)
                sData->teamCount++; // new mon added
            CTB_ClearIcon();
            sData->listCursor = sData->curTeamSlot;
            sData->listScroll = 0;
            CTB_EnterScreen(SCREEN_TEAM_OVERVIEW);
            PlaySE(SE_SELECT);
            break;

        default: break;
        }
    }
    // --- B button: go back / skip step ---
    else if (JOY_NEW(B_BUTTON))
    {
        switch (sData->screen)
        {
        case SCREEN_TEAM_OVERVIEW:
            // Exit without giving team
            CTB_ClearIcon();
            CTB_ClearTeamIcons();
            FreeMonIconPalettes();
            FreeAllWindowBuffers();
            Free(sData);
            sData = NULL;
            DestroyTask(taskId);
            ScriptContext_Enable();
            SetMainCallback2(CB2_ReturnToField);
            return;

        case SCREEN_SELECT_SPECIES:
            // Cancel add — go back to overview
            sData->listCursor = sData->teamCount;
            sData->listScroll = 0;
            CTB_EnterScreen(SCREEN_TEAM_OVERVIEW);
            PlaySE(SE_SELECT);
            break;

        case SCREEN_SELECT_MOVES:
            // Go to nature selection if species is set, or back to overview
            if (sData->team[sData->curTeamSlot].species != SPECIES_NONE)
                CTB_EnterScreen(SCREEN_SELECT_NATURE);
            else
            {
                sData->listCursor = sData->curTeamSlot;
                CTB_EnterScreen(SCREEN_TEAM_OVERVIEW);
            }
            PlaySE(SE_SELECT);
            break;

        case SCREEN_SELECT_NATURE:
            sData->curMoveSlot = 0;
            CTB_EnterScreen(SCREEN_SELECT_MOVES);
            PlaySE(SE_SELECT);
            break;

        case SCREEN_SELECT_ABILITY:
            CTB_EnterScreen(SCREEN_SELECT_NATURE);
            PlaySE(SE_SELECT);
            break;

        case SCREEN_SELECT_ITEM:
            CTB_EnterScreen(SCREEN_SELECT_ABILITY);
            PlaySE(SE_SELECT);
            break;

        case SCREEN_SELECT_TERA:
            sData->filterLetter = 0;
            CTB_PopulateItems();
            CTB_EnterScreen(SCREEN_SELECT_ITEM);
            PlaySE(SE_SELECT);
            break;

        case SCREEN_SET_EVS:
            CTB_EnterScreen(SCREEN_SELECT_TERA);
            PlaySE(SE_SELECT);
            break;

        case SCREEN_SET_IVS:
            sData->curStatIdx = 0;
            CTB_EnterScreen(SCREEN_SET_EVS);
            PlaySE(SE_SELECT);
            break;

        default: break;
        }
    }
    // --- START in overview: shortcut confirm ---
    else if (JOY_NEW(START_BUTTON) && sData->screen == SCREEN_TEAM_OVERVIEW
             && sData->teamCount > 0)
    {
        CTB_BuildAndExitTeam(taskId);
        return;
    }
}
