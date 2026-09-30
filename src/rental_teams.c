#include "global.h"
#include "main.h"
#include "bg.h"
#include "event_data.h"
#include "field_screen_effect.h"
#include "gpu_regs.h"
#include "list_menu.h"
#include "malloc.h"
#include "menu.h"
#include "menu_helpers.h"
#include "overworld.h"
#include "palette.h"
#include "party_menu.h"
#include "pokemon.h"
#include "pokemon_icon.h"
#include "random.h"
#include "rental_teams.h"
#include "scanline_effect.h"
#include "script.h"
#include "sound.h"
#include "sprite.h"
#include "string_util.h"
#include "strings.h"
#include "task.h"
#include "text.h"
#include "text_window.h"
#include "window.h"
#include "script_pokemon_util.h"
#include "constants/battle.h"
#include "constants/party_menu.h"
#include "constants/rgb.h"
#include "constants/songs.h"
#include "data/rental_teams.h"

#define TAG_RENTAL_SCROLL_ARROW 2100
#define ITEM_ID_RANDOM 1000

enum {
    WIN_HEADER,
    WIN_TEAM_LIST,
    WIN_PREVIEW,
    WIN_COUNT
};

static const u8 sText_CasualeHeader[] = _("CASUALE");
static const u8 sText_CasualeDescription[] = _("Premendo A\nriceverai un\nteam a sorpresa\ntra quelli di\nquesta lista!");
static const u8 sText_KeyHelp[] = _("A:Scegli  B:Esci");
static const u8 sText_AnteprimaHeader[] = _("ANTEPRIMA");
static const u8 sText_RandomTeamOption[] = _("  Team Casuale");
static const u8 sText_AllTeamsCategory[] = _("TUTTI I TEAM (76)");
static const u8 sText_HeaderPrefix[] = _("TEAM A NOLEGGIO - ");
static const u8 sText_UnknownTeam[] = _("Team Sconosciuto");

struct RentalTeamsMenuData
{
    u8 listTaskId;
    u8 scrollArrowsTaskId;
    u16 scrollOffset;
    u16 selectedRow;
    u8 monIconSpriteIds[6];
    u16 category;
    u16 numTeams;
    u16 numItems;
    struct ListMenuItem *menuItems;
};

static EWRAM_DATA struct RentalTeamsMenuData *sRentalTeamsData = NULL;

static const struct WindowTemplate sRentalTeamsWindowTemplates[] =
{
    [WIN_HEADER] = {
        .bg = 0,
        .tilemapLeft = 1,
        .tilemapTop = 1,
        .width = 28,
        .height = 2,
        .paletteNum = 15,
        .baseBlock = 0x0001,
    },
    [WIN_TEAM_LIST] = {
        .bg = 0,
        .tilemapLeft = 1,
        .tilemapTop = 3,
        .width = 18,
        .height = 16,
        .paletteNum = 15,
        .baseBlock = 0x0039,
    },
    [WIN_PREVIEW] = {
        .bg = 0,
        .tilemapLeft = 19,
        .tilemapTop = 3,
        .width = 10,
        .height = 16,
        .paletteNum = 15,
        .baseBlock = 0x0159,
    },
    DUMMY_WIN_TEMPLATE
};

static const struct BgTemplate sRentalTeamsBgTemplates[] =
{
    {
        .bg = 0,
        .charBaseIndex = 0,
        .mapBaseIndex = 31,
        .screenSize = 0,
        .paletteMode = 0,
        .priority = 1,
        .baseTile = 0,
    },
    {
        .bg = 1,
        .charBaseIndex = 2,
        .mapBaseIndex = 29,
        .screenSize = 0,
        .paletteMode = 0,
        .priority = 2,
        .baseTile = 0,
    },
};

static void VBlankCB_RentalTeams(void)
{
    LoadOam();
    ProcessSpriteCopyRequests();
    TransferPlttBuffer();
}

static void CB2_RentalTeamsMain(void)
{
    RunTasks();
    AnimateSprites();
    BuildOamBuffer();
    RunTextPrinters();
    DoScheduledBgTilemapCopiesToVram();
    UpdatePaletteFade();
}

static void FreeRentalTeamsResources(void)
{
    if (sRentalTeamsData != NULL)
    {
        if (sRentalTeamsData->menuItems != NULL)
            Free(sRentalTeamsData->menuItems);
        Free(sRentalTeamsData);
        sRentalTeamsData = NULL;
    }
    FreeAllWindowBuffers();
}

static void UpdateTeamPreview(s32 itemIndex)
{
    u8 i;
    for (i = 0; i < 6; i++)
    {
        if (sRentalTeamsData->monIconSpriteIds[i] != SPRITE_NONE)
        {
            FreeAndDestroyMonIconSprite(&gSprites[sRentalTeamsData->monIconSpriteIds[i]]);
            sRentalTeamsData->monIconSpriteIds[i] = SPRITE_NONE;
        }
    }

    FillWindowPixelBuffer(WIN_PREVIEW, PIXEL_FILL(1));

    if (itemIndex == ITEM_ID_RANDOM)
    {
        AddTextPrinterParameterized(WIN_PREVIEW, FONT_NORMAL, sText_CasualeHeader, 16, 2, TEXT_SKIP_DRAW, NULL);
        AddTextPrinterParameterized(WIN_PREVIEW, FONT_SMALL, sText_CasualeDescription, 4, 36, TEXT_SKIP_DRAW, NULL);
        AddTextPrinterParameterized(WIN_PREVIEW, FONT_SMALL, sText_KeyHelp, 4, 114, TEXT_SKIP_DRAW, NULL);
    }
    else if (itemIndex >= 0 && itemIndex < TOTAL_RENTAL_TEAMS)
    {
        AddTextPrinterParameterized(WIN_PREVIEW, FONT_NORMAL, sText_AnteprimaHeader, 12, 2, TEXT_SKIP_DRAW, NULL);

        static const s16 sIconCoords[6][2] = {
            { 176, 46 }, { 208, 46 },
            { 176, 74 }, { 208, 74 },
            { 176, 102 }, { 208, 102 },
        };

        for (i = 0; i < 6; i++)
        {
            u16 species = sRentalTeams[itemIndex].mons[i].species;
            if (species != SPECIES_NONE)
            {
                u8 spriteId = CreateMonIcon(species, SpriteCB_MonIcon, sIconCoords[i][0], sIconCoords[i][1], 0, 0);
                if (spriteId < MAX_SPRITES)
                {
                    gSprites[spriteId].oam.priority = 0;
                    sRentalTeamsData->monIconSpriteIds[i] = spriteId;
                }
            }
        }

        AddTextPrinterParameterized(WIN_PREVIEW, FONT_SMALL, sText_KeyHelp, 4, 114, TEXT_SKIP_DRAW, NULL);
    }

    CopyWindowToVram(WIN_PREVIEW, COPYWIN_GFX);
}

static void RentalTeams_MoveCursor(s32 itemIndex, bool8 onInit, struct ListMenu *list)
{
    PlaySE(SE_SELECT);
    UpdateTeamPreview(itemIndex);
}

static void Task_RentalTeams_FadeOutAndExit(u8 taskId)
{
    if (!gPaletteFade.active)
    {
        u8 i;
        for (i = 0; i < 6; i++)
        {
            if (sRentalTeamsData->monIconSpriteIds[i] != SPRITE_NONE)
            {
                FreeAndDestroyMonIconSprite(&gSprites[sRentalTeamsData->monIconSpriteIds[i]]);
                sRentalTeamsData->monIconSpriteIds[i] = SPRITE_NONE;
            }
        }
        if (sRentalTeamsData->scrollArrowsTaskId != TASK_NONE)
        {
            RemoveScrollIndicatorArrowPair(sRentalTeamsData->scrollArrowsTaskId);
            sRentalTeamsData->scrollArrowsTaskId = TASK_NONE;
        }
        DestroyListMenuTask(sRentalTeamsData->listTaskId, NULL, NULL);
        FreeMonIconPalettes();
        FreeRentalTeamsResources();
        DestroyTask(taskId);
        SetMainCallback2(CB2_ReturnToField);
    }
}

static void Task_RentalTeams_HandleInput(u8 taskId)
{
    if (gPaletteFade.active)
        return;

    if (JOY_NEW(SELECT_BUTTON))
    {
        PlaySE(SE_SELECT);
        u16 randIdx = Random() % sRentalTeamsData->numTeams;
        u16 teamId;
        if (sRentalTeamsData->category == CATEGORY_ALL)
            teamId = randIdx;
        else
            teamId = sRentalCategories[sRentalTeamsData->category].teamIndices[randIdx];

        gSpecialVar_Result = teamId;
        BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
        gTasks[taskId].func = Task_RentalTeams_FadeOutAndExit;
        return;
    }

    s32 input = ListMenu_ProcessInput(sRentalTeamsData->listTaskId);
    if (input == LIST_CANCEL)
    {
        PlaySE(SE_SELECT);
        gSpecialVar_Result = 0xFFFF;
        BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
        gTasks[taskId].func = Task_RentalTeams_FadeOutAndExit;
    }
    else if (input != LIST_NOTHING_CHOSEN)
    {
        PlaySE(SE_SELECT);
        if (input == ITEM_ID_RANDOM)
        {
            u16 randIdx = Random() % sRentalTeamsData->numTeams;
            u16 teamId;
            if (sRentalTeamsData->category == CATEGORY_ALL)
                teamId = randIdx;
            else
                teamId = sRentalCategories[sRentalTeamsData->category].teamIndices[randIdx];
            gSpecialVar_Result = teamId;
        }
        else
        {
            gSpecialVar_Result = input;
        }
        BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
        gTasks[taskId].func = Task_RentalTeams_FadeOutAndExit;
    }
}

static void BuildRentalTeamsList(void)
{
    u16 i;
    u16 cat = sRentalTeamsData->category;
    u16 count;

    if (cat == CATEGORY_ALL)
        count = TOTAL_RENTAL_TEAMS;
    else
        count = sRentalCategories[cat].count;

    sRentalTeamsData->numTeams = count;
    sRentalTeamsData->numItems = count + 1;

    sRentalTeamsData->menuItems = AllocZeroed(sizeof(struct ListMenuItem) * (sRentalTeamsData->numItems + 1));

    // First item is always Random Team
    sRentalTeamsData->menuItems[0].name = sText_RandomTeamOption;
    sRentalTeamsData->menuItems[0].id = ITEM_ID_RANDOM;

    for (i = 0; i < count; i++)
    {
        u16 teamId;
        if (cat == CATEGORY_ALL)
            teamId = i;
        else
            teamId = sRentalCategories[cat].teamIndices[i];

        sRentalTeamsData->menuItems[i + 1].name = sRentalTeams[teamId].listName;
        sRentalTeamsData->menuItems[i + 1].id = teamId;
    }

    gMultiuseListMenuTemplate.items = sRentalTeamsData->menuItems;
    gMultiuseListMenuTemplate.totalItems = sRentalTeamsData->numItems;
    gMultiuseListMenuTemplate.windowId = WIN_TEAM_LIST;
    gMultiuseListMenuTemplate.header_X = 0;
    gMultiuseListMenuTemplate.item_X = 8;
    gMultiuseListMenuTemplate.cursor_X = 0;
    gMultiuseListMenuTemplate.upText_Y = 1;
    gMultiuseListMenuTemplate.cursorPal = 2;
    gMultiuseListMenuTemplate.fillValue = 1;
    gMultiuseListMenuTemplate.cursorShadowPal = 3;
    gMultiuseListMenuTemplate.lettersSpacing = 0;
    gMultiuseListMenuTemplate.itemVerticalPadding = 0;
    gMultiuseListMenuTemplate.scrollMultiple = LIST_MULTIPLE_SCROLL_DPAD;
    gMultiuseListMenuTemplate.fontId = FONT_NORMAL;
    gMultiuseListMenuTemplate.cursorKind = CURSOR_BLACK_ARROW;
    gMultiuseListMenuTemplate.maxShowed = 7;
    gMultiuseListMenuTemplate.moveCursorFunc = RentalTeams_MoveCursor;
    gMultiuseListMenuTemplate.itemPrintFunc = NULL;
}

static void CB2_InitRentalTeams(void)
{
    switch (gMain.state)
    {
    case 0:
        ResetSpriteData();
        FreeAllSpritePalettes();
        ResetTasks();
        ClearScheduledBgCopiesToVram();
        SetVBlankCallback(VBlankCB_RentalTeams);
        gMain.state++;
        break;
    case 1:
        ResetVramOamAndBgCntRegs();
        ResetBgsAndClearDma3BusyFlags(0);
        InitBgsFromTemplates(0, sRentalTeamsBgTemplates, ARRAY_COUNT(sRentalTeamsBgTemplates));
        ResetAllBgsCoordinates();
        SetGpuReg(REG_OFFSET_DISPCNT, DISPCNT_MODE_0 | DISPCNT_OBJ_1D_MAP | DISPCNT_OBJ_ON);
        ShowBg(0);
        ShowBg(1);
        SetGpuReg(REG_OFFSET_BLDCNT, 0);

        InitWindows(sRentalTeamsWindowTemplates);
        DeactivateAllTextPrinters();
        LoadUserWindowBorderGfx(0, 0x0200, BG_PLTT_ID(14));
        LoadPalette(gStandardMenuPalette, BG_PLTT_ID(15), PLTT_SIZE_4BPP);

        DrawStdFrameWithCustomTileAndPalette(WIN_HEADER, FALSE, 0x0200, 14);
        DrawStdFrameWithCustomTileAndPalette(WIN_TEAM_LIST, FALSE, 0x0200, 14);
        DrawStdFrameWithCustomTileAndPalette(WIN_PREVIEW, FALSE, 0x0200, 14);

        PutWindowTilemap(WIN_HEADER);
        PutWindowTilemap(WIN_TEAM_LIST);
        PutWindowTilemap(WIN_PREVIEW);

        FillWindowPixelBuffer(WIN_HEADER, PIXEL_FILL(1));
        FillWindowPixelBuffer(WIN_TEAM_LIST, PIXEL_FILL(1));
        FillWindowPixelBuffer(WIN_PREVIEW, PIXEL_FILL(1));

        {
            u8 titleBuf[64];
            const u8 *catName;
            if (sRentalTeamsData->category == CATEGORY_ALL)
                catName = sText_AllTeamsCategory;
            else
                catName = sRentalCategories[sRentalTeamsData->category].name;

            StringCopy(titleBuf, sText_HeaderPrefix);
            StringAppend(titleBuf, catName);
            AddTextPrinterParameterized(WIN_HEADER, FONT_NORMAL, titleBuf, 6, 2, TEXT_SKIP_DRAW, NULL);
        }

        CopyWindowToVram(WIN_HEADER, COPYWIN_FULL);
        CopyWindowToVram(WIN_TEAM_LIST, COPYWIN_FULL);
        CopyWindowToVram(WIN_PREVIEW, COPYWIN_FULL);
        ScheduleBgCopyTilemapToVram(0);

        gMain.state++;
        break;
    case 2:
        LoadMonIconPalettes();
        gMain.state++;
        break;
    case 3:
        BuildRentalTeamsList();
        sRentalTeamsData->listTaskId = ListMenuInit(&gMultiuseListMenuTemplate, 0, 0);

        sRentalTeamsData->scrollArrowsTaskId = AddScrollIndicatorArrowPairParameterized(
            SCROLL_ARROW_UP,
            76,
            26,
            148,
            (sRentalTeamsData->numItems > 7) ? (sRentalTeamsData->numItems - 7) : 0,
            TAG_RENTAL_SCROLL_ARROW,
            TAG_RENTAL_SCROLL_ARROW,
            &sRentalTeamsData->scrollOffset
        );

        UpdateTeamPreview(sRentalTeamsData->menuItems[0].id);
        gMain.state++;
        break;
    case 4:
        SetBackdropFromColor(RGB(8, 12, 18));
        BeginNormalPaletteFade(PALETTES_ALL, 0, 16, 0, RGB_BLACK);
        CreateTask(Task_RentalTeams_HandleInput, 10);
        SetMainCallback2(CB2_RentalTeamsMain);
        break;
    }
}

static void Task_RentalTeams_WaitForFadeOut(u8 taskId)
{
    if (!gPaletteFade.active)
    {
        SetMainCallback2(CB2_InitRentalTeams);
        gFieldCallback = FieldCB_ContinueScriptHandleMusic;
        DestroyTask(taskId);
    }
}

void OpenRentalTeamBrowser(void)
{
    u16 cat = gSpecialVar_0x8004;
    if (cat > CATEGORY_ALL)
        cat = CATEGORY_ALL;

    sRentalTeamsData = AllocZeroed(sizeof(struct RentalTeamsMenuData));
    sRentalTeamsData->category = cat;
    sRentalTeamsData->scrollArrowsTaskId = TASK_NONE;

    u8 i;
    for (i = 0; i < 6; i++)
        sRentalTeamsData->monIconSpriteIds[i] = SPRITE_NONE;

    LockPlayerFieldControls();
    CreateTask(Task_RentalTeams_WaitForFadeOut, 10);
    BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
}

void ChooseRandomRentalTeam(void)
{
    u16 teamId = Random() % TOTAL_RENTAL_TEAMS;
    gSpecialVar_Result = teamId;
    StringCopy(gStringVar1, sRentalTeams[teamId].name);
}

void BufferRentalTeamName(void)
{
    u16 teamId = gSpecialVar_Result;
    if (teamId < TOTAL_RENTAL_TEAMS)
        StringCopy(gStringVar1, sRentalTeams[teamId].name);
    else
        StringCopy(gStringVar1, sText_UnknownTeam);
}

void GiveSelectedRentalTeam(void)
{
    u16 teamId = gSpecialVar_Result;
    u32 i, j;

    if (teamId >= TOTAL_RENTAL_TEAMS)
        return;

    ZeroPlayerPartyMons();

    for (i = 0; i < 6; i++)
    {
        const struct PresetRentalMon *rMon = &sRentalTeams[teamId].mons[i];
        if (rMon->species == SPECIES_NONE)
            continue;

        struct PokemonTemplate template = {0};
        template.species = rMon->species;
        template.heldItem = rMon->item;
        template.level = rMon->level;
        template.ball = ITEM_POKE_BALL;
        template.nature = rMon->nature;
        template.isShiny = rMon->isShiny;
        template.doNotUseDefaultShinyness = TRUE;
        template.abilityNum = rMon->abilityNum;
        template.doNotUseDefaultAbility = TRUE;
        template.teraType = rMon->teraType;
        template.doNotUseDefaultTeraType = (rMon->teraType != TYPE_NONE);
        template.ignoreTotalEvCheck = TRUE;

        for (j = 0; j < NUM_STATS; j++)
        {
            template.evs[j] = rMon->evs[j];
            template.ivs[j] = rMon->ivs[j];
        }

        for (j = 0; j < MAX_MON_MOVES; j++)
        {
            template.moves[j] = rMon->moves[j];
        }

        CreateMonFromTemplate(&gParties[B_TRAINER_PLAYER][i], &template);
    }

    CalculatePlayerPartyCount();
    HealPlayerParty();
}
