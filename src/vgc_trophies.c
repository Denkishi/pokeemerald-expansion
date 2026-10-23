#include "global.h"
#include "main.h"
#include "bg.h"
#include "event_data.h"
#include "field_screen_effect.h"
#include "gpu_regs.h"
#include "malloc.h"
#include "menu.h"
#include "menu_helpers.h"
#include "overworld.h"
#include "palette.h"
#include "sound.h"
#include "string_util.h"
#include "strings.h"
#include "task.h"
#include "text.h"
#include "text_window.h"
#include "window.h"
#include "constants/rgb.h"
#include "constants/songs.h"
#include "constants/vars.h"
#include "constants/opponents.h"
#include "vgc_trophies.h"

#define WIN_HEADER     0
#define WIN_TEAMS      1
#define WIN_CHECKLIST  2

#define VT_VISIBLE_TEAMS  7
#define VT_ROW_H          15
#define VT_FRAME_TILE     0x200
#define VT_FRAME_PAL      14
#define VT_TEXT_PAL       15

struct VgcTrophiesState
{
    u16 cursor;
    u16 scroll;
};

static EWRAM_DATA struct VgcTrophiesState sVT = {0};
static EWRAM_DATA u16 sVT_Tilemap[BG_SCREEN_SIZE / 2] = {0};

static const u8 sColDark[3]  = {TEXT_COLOR_WHITE, TEXT_COLOR_DARK_GRAY, TEXT_COLOR_LIGHT_GRAY};
static const u8 sColRed[3]   = {TEXT_COLOR_WHITE, TEXT_COLOR_RED,       TEXT_COLOR_LIGHT_GRAY};
static const u8 sColGreen[3] = {TEXT_COLOR_WHITE, TEXT_COLOR_GREEN,     TEXT_COLOR_LIGHT_GRAY};
static const u8 sColGray[3]  = {TEXT_COLOR_WHITE, TEXT_COLOR_LIGHT_GRAY, TEXT_COLOR_DARK_GRAY};

static const u8 sText_HeaderTitle[] = _("STORICO VIA GAMBARDELLA VGC");
static const u8 sText_DefeatedHeader[] = _("SCONFITTI");
static const u8 sText_HelpExit[] = _("{B_BUTTON} Esci");
static const u8 sText_ArrowUp[] = _("▲");
static const u8 sText_ArrowDown[] = _("▼");
static const u8 sText_AllComplete[] = _("★ 144/144!");
static const u8 sText_TotPrefix[] = _("TOT: ");
static const u8 sText_Slash144[] = _("/144");
static const u8 sText_Cursor[] = _("▶");
static const u8 sText_Slash12[] = _("/12");
static const u8 sText_Star[] = _("★");
static const u8 sText_ColonSpace[] = _(": ");
static const u8 sText_Dash[] = _("-");

static const u8 sVgcTeamNames[VGC_TEAMS_COUNT][16] =
{
    _("Enzo"),
    _("Simone"),
    _("Elia"),
    _("Rafel"),
    _("Tony"),
    _("Mario Lett"),
    _("Tatore"),
    _("Mario"),
    _("Gerozz"),
    _("Carletto"),
    _("Gigi Pinto"),
    _("Savarotto"),
};

static const u8 sVgcTrainerNames[VGC_TRAINERS_COUNT][16] =
{
    _("Enzo"),
    _("Simone"),
    _("Elia"),
    _("Rafel"),
    _("Tony"),
    _("Mario Lett"),
    _("Tatore"),
    _("Mario"),
    _("Gerozz"),
    _("Carletto"),
    _("Gigi Pinto"),
    _("Savarotto"),
};

static const struct BgTemplate sVT_BgTemplates[] =
{
    {
        .bg = 0,
        .charBaseIndex = 0,
        .mapBaseIndex = 31,
        .screenSize = 0,
        .paletteMode = 0,
        .priority = 0,
        .baseTile = 0,
    },
};

static const struct WindowTemplate sVT_WinTemplates[] =
{
    [WIN_HEADER] = {
        .bg = 0,
        .tilemapLeft = 1,
        .tilemapTop = 1,
        .width = 28,
        .height = 2,
        .paletteNum = VT_TEXT_PAL,
        .baseBlock = 0x0001,
    },
    [WIN_TEAMS] = {
        .bg = 0,
        .tilemapLeft = 1,
        .tilemapTop = 4,
        .width = 13,
        .height = 14,
        .paletteNum = VT_TEXT_PAL,
        .baseBlock = 0x0039,
    },
    [WIN_CHECKLIST] = {
        .bg = 0,
        .tilemapLeft = 15,
        .tilemapTop = 4,
        .width = 14,
        .height = 14,
        .paletteNum = VT_TEXT_PAL,
        .baseBlock = 0x00F0,
    },
    DUMMY_WIN_TEMPLATE
};

// ---------------------------------------------------------------------
// Gestione dati nel salvataggio
// ---------------------------------------------------------------------
bool32 GetVgcTrophy(u32 teamIdx, u32 trainerIdx)
{
    u32 bit;
    u16 varId;
    u16 val;

    if (teamIdx >= VGC_TEAMS_COUNT || trainerIdx >= VGC_TRAINERS_COUNT)
        return FALSE;

    bit = teamIdx * VGC_TRAINERS_COUNT + trainerIdx;
    varId = VAR_VGC_TROPHIES_START + (bit / 16);
    val = VarGet(varId);
    return (val & (1 << (bit % 16))) != 0;
}

void SetVgcTrophy(u32 teamIdx, u32 trainerIdx)
{
    u32 bit;
    u16 varId;
    u16 val;

    if (teamIdx >= VGC_TEAMS_COUNT || trainerIdx >= VGC_TRAINERS_COUNT)
        return;

    bit = teamIdx * VGC_TRAINERS_COUNT + trainerIdx;
    varId = VAR_VGC_TROPHIES_START + (bit / 16);
    val = VarGet(varId);
    val |= (1 << (bit % 16));
    VarSet(varId, val);
}

u32 GetVgcTeamTrophiesCount(u32 teamIdx)
{
    u32 i;
    u32 count = 0;

    for (i = 0; i < VGC_TRAINERS_COUNT; i++)
    {
        if (GetVgcTrophy(teamIdx, i))
            count++;
    }
    return count;
}

u32 GetTotalVgcTrophiesCount(void)
{
    u32 t;
    u32 total = 0;

    for (t = 0; t < VGC_TEAMS_COUNT; t++)
        total += GetVgcTeamTrophiesCount(t);
    return total;
}

// ---------------------------------------------------------------------
// Disegno dello schermo
// ---------------------------------------------------------------------
static void VT_DrawHeader(void)
{
    u32 total = GetTotalVgcTrophiesCount();

    FillWindowPixelBuffer(WIN_HEADER, PIXEL_FILL(TEXT_COLOR_WHITE));
    AddTextPrinterParameterized3(WIN_HEADER, FONT_SMALL, 4, 1, sColDark, TEXT_SKIP_DRAW, sText_HeaderTitle);

    if (total >= VGC_TEAMS_COUNT * VGC_TRAINERS_COUNT)
    {
        AddTextPrinterParameterized3(WIN_HEADER, FONT_SMALL, 150, 1, sColRed, TEXT_SKIP_DRAW, sText_AllComplete);
    }
    else
    {
        ConvertIntToDecimalStringN(gStringVar1, total, STR_CONV_MODE_LEFT_ALIGN, 3);
        StringCopy(gStringVar4, sText_TotPrefix);
        StringAppend(gStringVar4, gStringVar1);
        StringAppend(gStringVar4, sText_Slash144);
        AddTextPrinterParameterized3(WIN_HEADER, FONT_SMALL, 170, 1, sColRed, TEXT_SKIP_DRAW, gStringVar4);
    }

    CopyWindowToVram(WIN_HEADER, COPYWIN_GFX);
}

static void VT_DrawTeams(void)
{
    u32 i;

    FillWindowPixelBuffer(WIN_TEAMS, PIXEL_FILL(TEXT_COLOR_WHITE));

    for (i = 0; i < VT_VISIBLE_TEAMS; i++)
    {
        u32 teamIdx = sVT.scroll + i;
        u32 y = i * VT_ROW_H + 3;
        u32 count;
        const u8 *nameCol;

        if (teamIdx >= VGC_TEAMS_COUNT)
            break;

        count = GetVgcTeamTrophiesCount(teamIdx);

        if (teamIdx == sVT.cursor)
        {
            AddTextPrinterParameterized3(WIN_TEAMS, FONT_NORMAL, 1, y - 2, sColRed, TEXT_SKIP_DRAW, sText_Cursor);
            nameCol = sColRed;
        }
        else
        {
            nameCol = sColDark;
        }

        AddTextPrinterParameterized3(WIN_TEAMS, FONT_SMALL, 8, y, nameCol, TEXT_SKIP_DRAW, sVgcTeamNames[teamIdx]);

        ConvertIntToDecimalStringN(gStringVar1, count, STR_CONV_MODE_RIGHT_ALIGN, 2);
        StringCopy(gStringVar4, gStringVar1);
        StringAppend(gStringVar4, sText_Slash12);

        if (count == VGC_TRAINERS_COUNT)
        {
            AddTextPrinterParameterized3(WIN_TEAMS, FONT_SMALL, 68, y, sColGreen, TEXT_SKIP_DRAW, sText_Star);
            AddTextPrinterParameterized3(WIN_TEAMS, FONT_SMALL, 76, y, sColGreen, TEXT_SKIP_DRAW, gStringVar4);
        }
        else
        {
            AddTextPrinterParameterized3(WIN_TEAMS, FONT_SMALL, 76, y, sColGray, TEXT_SKIP_DRAW, gStringVar4);
        }
    }

    if (sVT.scroll > 0)
        AddTextPrinterParameterized3(WIN_TEAMS, FONT_SMALL, 95, 0, sColRed, TEXT_SKIP_DRAW, sText_ArrowUp);
    if (sVT.scroll + VT_VISIBLE_TEAMS < VGC_TEAMS_COUNT)
        AddTextPrinterParameterized3(WIN_TEAMS, FONT_SMALL, 95, VT_ROW_H * (VT_VISIBLE_TEAMS - 1) + 2, sColRed, TEXT_SKIP_DRAW, sText_ArrowDown);

    CopyWindowToVram(WIN_TEAMS, COPYWIN_GFX);
}

static void VT_DrawChecklist(void)
{
    u32 i;
    u32 teamWins = GetVgcTeamTrophiesCount(sVT.cursor);

    FillWindowPixelBuffer(WIN_CHECKLIST, PIXEL_FILL(TEXT_COLOR_WHITE));

    // Intestazione con conteggio
    ConvertIntToDecimalStringN(gStringVar1, teamWins, STR_CONV_MODE_LEFT_ALIGN, 2);
    StringCopy(gStringVar4, sText_DefeatedHeader);
    StringAppend(gStringVar4, sText_ColonSpace);
    StringAppend(gStringVar4, gStringVar1);
    StringAppend(gStringVar4, sText_Slash12);
    AddTextPrinterParameterized3(WIN_CHECKLIST, FONT_NORMAL, 4, 1, (teamWins == VGC_TRAINERS_COUNT) ? sColGreen : sColDark, TEXT_SKIP_DRAW, gStringVar4);

    // 2 Colonne da 6 avversari
    for (i = 0; i < VGC_TRAINERS_COUNT; i++)
    {
        u32 col = i / 6;
        u32 row = i % 6;
        u32 x = (col == 0) ? 4 : 57;
        u32 y = 16 + row * 13;
        bool32 beaten = GetVgcTrophy(sVT.cursor, i);

        if (beaten)
        {
            AddTextPrinterParameterized3(WIN_CHECKLIST, FONT_SMALL, x, y, sColGreen, TEXT_SKIP_DRAW, sText_Star);
            AddTextPrinterParameterized3(WIN_CHECKLIST, FONT_SMALL, x + 8, y, sColDark, TEXT_SKIP_DRAW, sVgcTrainerNames[i]);
        }
        else
        {
            AddTextPrinterParameterized3(WIN_CHECKLIST, FONT_SMALL, x, y, sColGray, TEXT_SKIP_DRAW, sText_Dash);
            AddTextPrinterParameterized3(WIN_CHECKLIST, FONT_SMALL, x + 8, y, sColGray, TEXT_SKIP_DRAW, sVgcTrainerNames[i]);
        }
    }

    // Aiuto tasti in basso
    AddTextPrinterParameterized3(WIN_CHECKLIST, FONT_SMALL, 4, 98, sColRed, TEXT_SKIP_DRAW, sText_HelpExit);

    CopyWindowToVram(WIN_CHECKLIST, COPYWIN_GFX);
}

static void VT_DrawAll(void)
{
    VT_DrawHeader();
    VT_DrawTeams();
    VT_DrawChecklist();
    CopyBgTilemapBufferToVram(0);
}

// ---------------------------------------------------------------------
// Loop Principale & Task Input
// ---------------------------------------------------------------------
static void Task_VT_ExitToField(u8 taskId)
{
    if (gPaletteFade.active)
        return;

    DestroyTask(taskId);
    SetVBlankCallback(NULL);
    ResetSpriteData();
    FreeAllSpritePalettes();
    ClearScheduledBgCopiesToVram();
    FreeAllWindowBuffers();
    UnsetBgTilemapBuffer(0);

    gFieldCallback = FieldCB_ContinueScriptHandleMusic;
    SetMainCallback2(CB2_ReturnToField);
}

static void Task_VT_Input(u8 taskId)
{
    if (gPaletteFade.active)
        return;

    if (JOY_NEW(DPAD_UP))
    {
        if (sVT.cursor > 0)
        {
            sVT.cursor--;
            if (sVT.cursor < sVT.scroll)
                sVT.scroll = sVT.cursor;
            PlaySE(SE_SELECT);
            VT_DrawTeams();
            VT_DrawChecklist();
        }
    }
    else if (JOY_NEW(DPAD_DOWN))
    {
        if (sVT.cursor + 1 < VGC_TEAMS_COUNT)
        {
            sVT.cursor++;
            if (sVT.cursor >= sVT.scroll + VT_VISIBLE_TEAMS)
                sVT.scroll = sVT.cursor - VT_VISIBLE_TEAMS + 1;
            PlaySE(SE_SELECT);
            VT_DrawTeams();
            VT_DrawChecklist();
        }
    }
    else if (JOY_NEW(B_BUTTON | A_BUTTON))
    {
        PlaySE(SE_SELECT);
        BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
        gTasks[taskId].func = Task_VT_ExitToField;
    }
}

static void CB2_VT_Main(void)
{
    RunTasks();
    AnimateSprites();
    BuildOamBuffer();
    UpdatePaletteFade();
}

static void VBlankCB_VT(void)
{
    LoadOam();
    ProcessSpriteCopyRequests();
    TransferPlttBuffer();
}

static void CB2_VT_Init(void)
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
        ClearScheduledBgCopiesToVram();
        gMain.state++;
        break;
    case 2:
        ResetBgsAndClearDma3BusyFlags(0);
        InitBgsFromTemplates(0, sVT_BgTemplates, ARRAY_COUNT(sVT_BgTemplates));
        memset(sVT_Tilemap, 0, sizeof(sVT_Tilemap));
        SetBgTilemapBuffer(0, sVT_Tilemap);
        ChangeBgX(0, 0, BG_COORD_SET);
        ChangeBgY(0, 0, BG_COORD_SET);
        InitWindows(sVT_WinTemplates);
        DeactivateAllTextPrinters();
        gMain.state++;
        break;
    case 3:
        LoadUserWindowBorderGfx(WIN_HEADER, VT_FRAME_TILE, BG_PLTT_ID(VT_FRAME_PAL));
        LoadPalette(gStandardMenuPalette, BG_PLTT_ID(VT_TEXT_PAL), PLTT_SIZE_4BPP);
        gMain.state++;
        break;
    case 4:
        DrawStdFrameWithCustomTileAndPalette(WIN_HEADER, FALSE, VT_FRAME_TILE, VT_FRAME_PAL);
        DrawStdFrameWithCustomTileAndPalette(WIN_TEAMS, FALSE, VT_FRAME_TILE, VT_FRAME_PAL);
        DrawStdFrameWithCustomTileAndPalette(WIN_CHECKLIST, FALSE, VT_FRAME_TILE, VT_FRAME_PAL);
        VT_DrawAll();
        gMain.state++;
        break;
    default:
        ShowBg(0);
        SetGpuReg(REG_OFFSET_DISPCNT, DISPCNT_OBJ_ON | DISPCNT_OBJ_1D_MAP | DISPCNT_BG0_ON);
        SetGpuReg(REG_OFFSET_BLDCNT, 0);
        SetBackdropFromColor(RGB(8, 12, 18));
        BeginNormalPaletteFade(PALETTES_ALL, 0, 16, 0, RGB_BLACK);
        SetVBlankCallback(VBlankCB_VT);
        SetMainCallback2(CB2_VT_Main);
        CreateTask(Task_VT_Input, 0);
        break;
    }
}

static void Task_VT_WaitFadeAndOpen(u8 taskId)
{
    if (gPaletteFade.active)
        return;
    DestroyTask(taskId);
    CleanupOverworldWindowsAndTilemaps();
    SetMainCallback2(CB2_VT_Init);
}

void OpenVgcTrophiesScreen(void)
{
    memset(&sVT, 0, sizeof(sVT));
    LockPlayerFieldControls();
    CreateTask(Task_VT_WaitFadeAndOpen, 10);
    BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
}
